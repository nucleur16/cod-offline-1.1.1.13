# Selectable HUD styles (`k33_hudStyle`)

The combat HUD of 1.1.1.13 can show the layout of a later Call of Duty Online build (2.6.9.7 or 3.3.3.3), or SM2, an
original HUD written as `.menu` source. The style switches live.

| `k33_hudStyle` | id | build | state |
|---|---|---|---|
| 0 | `stock` | 1.1.1.13 | the game's own HUD |
| 1 | `v12` | 1.2.1.12 | not shipped: a visual duplicate of v26 (see below) |
| 2 | `v26` | 2.6.9.7 | shipped |
| 3 | `v33` | 3.3.3.3 | shipped (its own operators read from its executable, see "3.3.3.3") |
| 4 | `v35` | 3.5.2.6 | not shipped: no unpacked 3.5.2.6 executable exists on disk (see below) |
| 5 | `sm2` | authored | shipped: an original HUD laid out after a screenshot of the cancelled SM2 mod's HUD, written in `.menu` source (see "SM2") |

`k33_hudStyleList` in this build: `0:stock;2:v26;3:v33;5:sm2`. SM2 takes 5, not 4: the numbers are fixed, and 4
stays reserved for 3.5.2.6 as 1 does for 1.2.1.12.

Runtime-checked on 2026-11-12 in the offline fork's client (own test folder, Team Deathmatch on Crash, 1920x1080
window): see "Live check". The static evidence is what the generator and the tests check without a client.

## How it works

The weapon bar, score bar, minimap, killstreak column and perk display of every CODOL build are native menus of
`ui_mp/hud_720.txt` (menu assets, type 26), not Scaleform. A style is therefore menu data:

- `tools/build_hud_styles.py` converts the items of a later build's HUD menus to the 1.1.1.13 layout and appends
  them to 1.1.1.13's own menu of the same name. The stock items stay, each behind
  `dvarint("k33_hudStyle") != 2 && {its own rule}`; a style's items sit behind
  `dvarint("k33_hudStyle") == N && {their own rule}`. The menu's own visibility becomes
  `(stock style && {1.1.1.13's rule}) || (style 2 && {2.6.9.7's rule}) || (style 3 && {3.3.3.3's rule})`.
  `{..}` is a whole statement used as one operand, the form the stock menus use for their sub-expressions, so no
  wrapped expression is re-parenthesised.
- C1 evaluates these expressions every frame. `set k33_hudStyle 2` therefore changes the HUD in the same frame:
  no menu reload, no vid_restart, no patch in C1. A value that is not a shipped style shows the stock items, so
  the HUD is never empty.
- `src/hud_style.c` owns the dvars: `k33_hudStyle` (int 0..5, archived, registered once `C1DvarsReady()`, a
  command-line `+set k33_hudStyle N` re-issued as voice.c does) and `k33_hudStyleList` (string). A saved value that
  is not in the list is set back to 0 and logged once. It also implements what the later HUD reads and 1.1.1.13
  lacks ("Implemented natively").

### One copy of each menu

C1 keeps the first menu it links under a name (docs/SURVIVAL_HUD_20261029.md: `Load_MenuDefPtr` 0x4C17B0 ->
0x4CAFF0 -> `DB_LinkXAssetEntry`). `survival_hud_postgfx_mp` already replaces `killstreaks_hd` and `scorebar_hd`
to hide them in Survival, so those two menus are not shipped a second time: `tools/build_survival_hud.py`
`styled()` passes its own authoring through `fold_survival()`, and the survival zone's menufile carries both
gates. `hud_styles_postgfx_mp` carries the two menus the survival zone does not own:

| menu | zone that carries it | items: stock + v26 + v33 |
|---|---|---|
| `weaponbar_hd` | hud_styles_postgfx_mp | 25 + 30 + 30 |
| `minimap_fullscreen` | hud_styles_postgfx_mp | 16 + 17 + 17 |
| `killstreaks_hd` | survival_hud_postgfx_mp | 69 + 83 + 121 |
| `scorebar_hd` | survival_hud_postgfx_mp | 33 (+4 Survival panel) + 43 + 62 |

Survival behaves as before in every style: `killstreaks_hd` is hidden by its menu, the stock score items by each
item, and the Survival panel's four items do not depend on the style. A style's own score-panel items carry
`g_gametype != "survival"` as well (they are MP items, like the stock ones); the kill/death bar keeps the later
build's own rule, which shows it in Survival.

Both zones are parts of `codol_common_postgfx_mp` (after `code_post_gfx`, permanent). `hud_styles_postgfx_mp`
is listed ahead of `survival_hud_postgfx_mp`, so the images and materials are linked before either menufile
names them.

### Menus 1.1.1.13 does not have

`weaponbar_hd_extend`, `reloadbar` and `kd_rate_bar` (and 3.3.3.3's `extra_info` and `killstreaks_hd2`) exist only in
the later builds. 1.1.1.13 draws the menus its
own `hud_720.txt` lists, so their items join a menu it does draw (`weaponbar_hd`, `weaponbar_hd`, `scorebar_hd`;
`scorebar_hd` and `killstreaks_hd` for 3.3.3.3's two),
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

| 3.3.3.3 menu | kept | lands in |
|---|---|---|
| `weaponbar_hd` | 26 of 26 | weaponbar_hd |
| `weaponbar_hd_extend` | 1 of 4 | weaponbar_hd |
| `reloadbar` | 1 of 1 | weaponbar_hd |
| `scorebar_hd` | 32 of 59 (17 Survival / Evac, 10 co-op gunner) | scorebar_hd |
| `kd_rate_bar` | 21 of 25 (the revives caption and value, twice) | scorebar_hd |
| `extra_info` (team juggernaut / danger captions) | 9 of 9 | scorebar_hd |
| `killstreaks_hd2` (streak segments, slot plates, key hints) | 121 of 124 (the three ace-streak items) | killstreaks_hd |
| `minimap_fullscreen` | 17 of 17 | minimap_fullscreen |

3.3.3.3's `exp_bar` and `perk_sidebar_hud` are not hosted: 1.1.1.13 draws no XP bar on its combat HUD, and the perk
sidebar reads the timed-perk system (below).

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
   `hud style: k33_hudStyle registered (k33_hudStyleList 0:stock;2:v26;3:v33;5:sm2), value 0`.
3. In a private Team Deathmatch, write `set k33_hudStyle 2` (then 3, then 5) into `<exe dir>\k33_cmd_<pid>.txt` (or use the pause
   menu's HUD style control): the weapon bar, score bar, minimap frame and killstreak column switch to the
   2.6.9.7 (3.3.3.3) layout within half a second, without a reload. `set k33_hudStyle 0`: stock. Fire a few rounds: the
   big clip count and the gauge drop in the same frame as the reserve; reload refills both. With an akimbo weapon
   the clip count is both hands.
4. `set k33_hudStyle 1` (or 4): the stock HUD, the dvar back at 0, one line
   `hud style: k33_hudStyle 1 is not in this build (0:stock;2:v26;3:v33;5:sm2); showing the stock HUD`.
5. Quit and start again: the last shipped value is still set (archived).
6. Repeat step 3 in Survival: the enemy / credit panel is the same in all three, the MP score panel and the
   killstreak column stay hidden.

## 3.3.3.3 (v33) and 3.5.2.6 (v35)

### Operator numbering

Inferred first by aligning the menus each build shares with 2.6.9.7 (the same statement place, entry count and
operands give one operator pair per position; `scratchpad .../scripts/opalign.py`), then confirmed for 3.3.3.3 by
its evaluator, which is readable (3.3.3.3's `codoMP_client_shipRetail.exe` is not packed; it only has no operator
name table):

- 3.3.3.3: the switch at 0x00575DF1 (`lea eax, [ecx - 0x1E]; cmp eax, 0xF4; jmp [eax*4 + 0x00578090]`, 245 cases).
  Identical to 2.6.9.7 up to 97 (`spectatingfree`), two operators inserted at 98 / 99, 2.6.9.7 + 2 from
  `keybinding` (107) through `getweaponfiretype` (241), then 3.x's own 242..273 (`OPS_3333`).
- 3.5.2.6 (from the menu alignment only): identical up to 96, +1 at 97, +8 from `keybinding` (113).

### What 3.3.3.3's own operators do, and what they become

Each case was disassembled (`v33bin/ops242.txt` and the helpers it calls in the scratch folder). A constant is
exact in 1.1.1.13 when the state the case reads cannot exist there.

| op | case (helper) | what it does in 3.3.3.3 | in 1.1.1.13 |
|---|---|---|---|
| 242..252 | 0x577C7F.. (0x572240 / 0x5722A0 / 0x572310 ..) | per-squad-member records of the Survival / Evac / co-op-gunner panels (exists, health float, two byte fields, a dword, timers, text width) | their items are dropped: Survival / Evac panels (Kernel33's own survival panel stays) |
| 253 | 0x577C0D | mapname starts with `coop_gun_` | 0; the ten co-op-gunner items it gates are dropped as never shown |
| 254 | 0x577E68 (0x574DC0) | XP fraction of the XP bar | not used by a hosted menu (`exp_bar` only) |
| 255 | 0x576F85 (0x570BC0) | killstreak slot N's glow 0..100, an animation that rises when the slot becomes usable and falls back | 100, its resting value (the glow animation is not reproduced) |
| 256 | 0x577153 (0x573440) | two int arguments (count, empty): the segment material `hud_killstreak_bar_{empty,full}_{2,1}`, `_2` for a count of 3 or less | `select((A) <= 3, .._2, .._1)` with C1's own operators, the empty/full choice folded (B is a constant in every use) |
| 257, 258, 270 | 0x574C60 / 0x574CF0 / 0x574D60 | only for gametypes `asym*` / `borg*` (robot-axis flag, cg + 0x3508), else 0 | 0 |
| 259..263, 266, 268, 269, 272 | 0x5716E0 .. 0x5758A0 | not used by a hosted menu | - |
| 264 | 0x577EA1 | `g_gametype` starts with `asym` | 0 |
| 265 | 0x571D80 | playerdata `aarRoundNumber` + 1; every use is under 264 | 1 |
| 267 | 0x5719B0 | the team juggernaut's name; every use is under `g_gametype == "tjugg"` | "" |
| 271 | 0x575810 | the ace streak's icon when playerdata `acestreak` > 0 | its item is dropped with the ace-streak items (`getplayerdata("acestreak")`: a 3.x streak no 1.1.1.13 menu reads) |
| 273 | 0x575910 | weaponDef + 0x47C of the weapon in hand; its one use is `|| ui_gametype != "clash"` | 0 (any value gives the same result: 1.1.1.13 has no clash mode) |

`tests/test_hud_styles.py` `test_v33_own_operators` checks that no 3.x operator is left in the built menus and
that each used one went through its rewrite.

### Ownerdraws

3.3.3.3's `CG_OwnerDraw` (switch at 0x0044D8A8, byte table 0x0044E3B0, jump table 0x0044E294) against C1's
(0x00459E10): the hosted menus use the same numbers as 2.6.9.7's (5, 83, 103, 104, 120, 150..175, 284, 294, 297).
Its ammo display (ownerDraw 5, 0x004490A0) has the same layout constants as 1.2.1.12's, so the same three items
replace it; its one addition, a heat value instead of the clip for weapons with weaponDef + 0x47C == 5, has no
weapon in 1.1.1.13. 297 (the zombies charge attack) stays out as for v26.

### Materials

3.3.3.3's materials are 0x68 bytes with the name at +0x10, the texture count at +0x50, the technique set at +0x58
and the texture table at +0x5C (measured on common_mp; CodolSoundDumper `zone-materials` reads both layouts). The
four segment materials are in `common_marketplace_lite`, not common_mp. 12 images are carried as `k33hud33_*`;
the others are pixel-equal to v26's and share its `k33hud26_*` assets (weapon bar frame, score bars, kill/death
plate, minimap ping). The score bars use the same white art tinted with 1.1.1.13's team colours as v26.

### 3.5.2.6

Not shipped. Its executable is packed and no unpacked 3.5.2.6 image is on disk (only 1.1.1.x databases were
found), so its own operators (from 3.5.2.6 + 8 onward; menu visibility of its weapon bar uses one) cannot be read.

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

## SM2 (k33_hudStyle 5): an authored style in `.menu` source

SM2 is not converted from another build. It is an original HUD written as IW-style menu source,
`zones/hud_styles_postgfx_mp/sources/sm2.menu`, laid out after a screenshot of the cancelled SM2 mod's HUD that the
user supplied. No art is copied: every item draws `white`, a material 1.1.1.13 already loads, or a 1.1.1.13 / Kernel33
ownerdraw.

- **Compiler.** `zonekit/menu_source.py` reads the subset of the IW4 menu language a HUD needs (`#define`,
  `menuDef { name, visible when, stockItems }`, and `itemDef` with `rect`, `type`, `style`, `ownerdraw`,
  `background`, colours, text fields, `text`, `visible when`, `exp text / material / rect X|Y|W|H /
  forecolor|backcolor R|G|B|A`). Expressions become the infix entry stream C1 evaluates; function names must be
  operators of 1.1.1.13's table, and anything else is a compile error.
- **Build.** `STYLES["sm2"]["menu"]` marks an authored style. `authored_parts()` turns each menuDef into the part of
  its host: the authored items, then the host's own items that `stockItems` names, unchanged. Its visibility is the
  menuDef's or, without one, the host's. From there it is gated like v26 / v33: the stock items get `!= 5` and the
  SM2 items `== 5`. An authored style must have all four hosts. An item 1.1.1.13 cannot draw stops the build
  (`check_authored`): an unknown ownerdraw, a material not drawn by the stock HUD or not in `AUTHORED_MATERIALS`, a
  `player()` field C1 lacks, or an unknown text.
- **What it shows** (positions in the hosts' 640x480 units):
  - bottom right, a dark translucent panel: the large clip count (ownerDraw 3001, akimbo-aware), a thin divider,
    the grey reserve (`player("stockAmmo")`) and the weapon's icon (ownerDraw 3004, below);
  - above the panel, the weapon name (ownerDraws 81 / 83, as the stock HUD: it shows on a weapon switch and in
    killcam);
  - above that, a small dark box with the lethal grenade and its count, and the tactical (or smoke / flash) one
    while carried;
  - the killstreaks on the right edge: three dark slots, always drawn, bottom to top the chosen streaks 1-3
    (`player("killstreakID1..3")`). Each icon is `tablelookup("mp/coKillstreakStatsTable.csv", 0, id, 17)`,
    the stock killstreak menu's own expression. It is dim (alpha 0.3) while `player("killstreak")` is below the
    streak's `player("killstreakTargetN")`, and lit once `actionslotusable(N + 1)`, with a white top line and its
    key (`keybinding("+actionslot N + 1")`) in yellow. `actionslothudiconoverride` is the icon when no streak id
    is set, as in the stock menu. A fourth slot above appears only while a fourth streak is chosen; the support
    item of action slot 1 sits left of the first slot while it can be called in;
  - the streak counter beside the slots: one pip per kill up to `player("killstreakTarget")`, lit up to
    `player("killstreak")` (the stock column's pips, 25 at most). 1.1.1.13's own column is no longer drawn
    under SM2: it sat under the SM2 panels;
  - top left, the minimap on a dark square, with a compass strip over its top edge: ownerDraw 3005 (below), the heading tape
    1.1.1.13's own killstreak HUD draws with `viper_heading_mp` (common_mp);
  - top centre: own score, a slim bar filled from its inner end in blue, the side's emblem
    (`g_TeamIcon_Allies` / `g_TeamIcon_Axis`, hidden in free-for-all), the enemy bar filled from its inner end in
    red, the enemy score; under it the match state (`MPUI_WINNING_CAPS` / `LOSING` / `TIED`, coloured) and a small
    round timer (1.1.1.13's own expression);
  - bottom left, the player's name (`dvarstring("name")`) over the health bar: a dark track and the fill
    (ownerDraw 3003, below), red below 30 %;
  - 1.1.1.13's half-time / overtime captions, bomb and nuke timers and Sabotage captions (stock items 16-24, 28-32 of
    `scorebar_hd`) and its low-ammo warning are kept.
- **Ownerdraws.**
  - 145, the heading tape (`PROBED_OWNERDRAWS`: none of the four hosts' stock items uses it). A stock common_mp
    menu draws it as a 400x20 tape with `viper_heading_mp`, found by scanning common_mp's menu assets for
    ownerDraw 145. Live: it scrolls with the view.
  - 3005, the compass strip (`hud_style.c` `HudStyleTape`), which replaced 145 in sm2.menu on 2026-11-15. It calls
    C1's tape (0x004321B0, ownerDraw 145's) with three changes, each for its own call only and put back after:
    - **No objectives** (16 November, user request): the tape's last argument is 0, as ownerDraw 146 passes. The
      tape then returns after drawing the material (0x0043237E `cmp byte [ebp+20h], 0` / `je 0x0043273D`): no icons and
      no distances ("%im" / "%.1fm", which under SM2 read as the minimap's).
    - **The stretch**: `viper_heading_mp` carries 720 degrees (two laps) across its width and dvar 249
      compassTickertapeStretch (0.5) shows half of it, a whole lap in 100 units; the strip shows 120 degrees
      (`HudTapeStretch(120, 720)`), so the labels keep their size.
    - **The heading's rate** (16 November, user report: the letters came in the wrong order): the tape centres the
      material on -(cg + 0xB441C view yaw - cg + 0xBFA08 compass north) / 360 (0x004322B3, byte-checked), one material width per
      turn; with two laps in that width the labels went round twice per turn (facing east showed S, facing south N).
      3005 writes cg + 0xBFA08 = `HudTapeNorth()` = (yaw + north) / 2 for its call, which halves the difference the
      tape reads: one lap of labels per turn. Nothing else reads 0xBFA08 during the call: with objectives 0 the tape
      reads it once (0x004322B9); its other readers (the compass draws 0x00430ACB..0x00436676, written at 0x0043DDD9
      / 0x00450E34) run outside it, on the same thread, after the value is restored. The cg is the tape's own
      ([0x016D3B58 + [0x06C3D2AC] * 4] ^ 0x490B400C at 0x004321EB..0x0043220E, `ParityCg`).
    Live (16 November, co-op on mp_crash_tropical, headless, `compassRotation` (dvar 245) 0 so the minimap is
    north-up and its arrow is the ground truth; turned with the server's `dev_clientcmd 0 setviewpos 600 400 200 <yaw>`):
    yaw 0 arrow up, strip centred on N; 90 arrow left, W; 180 arrow down, S; 270 arrow right, E; 45 arrow up-left,
    between 30 and 33 (315). Before the change the same build drew S facing east. Objective icons: not seen live
    (co-op on Crash has no compass objectives); the skip is the tape's own branch above.
  - 3003, the health fill (`hud_style.c`). The menus cannot read health: `player()` has no field and no dvar
    holds it. C1's own health bar, ownerDraw 79 (`CG_DrawPlayerBarHealth` 0x004558C0), returns early unless a
    dvar ([0x016D0F4C] + 0xC) is set, so it drew nothing. 3003 reads the same fraction C1's bar computes
    (0x00453EE0: cg + 0x190 health over cg + 0x198 max health, clamped, 0 for pm type 8; byte-checked from
    0x00453EE6) when the item is drawn, so it moves in the same frame. It fills the item's rect from its left
    end, in the item's colour, red (0.9 0.22 0.2) below 30 % (`hud_style_policy.h HudHealthPermille`).
  - 3004, the weapon icon (`hud_style.c`). C1's own ownerDraw 118 (`CG_DrawPlayerWeaponIcon` 0x0042B960 ->
    0x0042B860) stretches the icon to the weapon def's `hudIconRatio` (+0x554; 1: x -= w, w *= 2; 2: x -= 3w,
    w *= 4). Live, the Vector's icon is a 128x64 image under a 4:1 ratio, so 118 drew it twice as wide. 3004
    draws the def's HUD icon (+0x550, byte-checked at 0x0042B9D9) as high as the rect and as wide as the
    image's own width / height, at most the rect, right-aligned (`HudIconFit`). The image size is the
    material's first texture's GfxImage width / height (Material texture table +0x54, MaterialTextureDef image
    +8, GfxImage +0x14 / +0x16). A def without a HUD icon (the sniper rifles drew nothing) takes the icon C1
    itself falls back to for its per-weapon icon table: +0x548, else +0x540 (0x0047C4D7, byte-checked). Each
    new icon is logged with its material name and image size.
- **Not possible as menus:** a constant weapon name. There is no menu value with the localized name:
  `weaponname()` is the internal name (`kriss_mp`), so the name shows when C1 shows it (on a switch, in
  killcam).
- **C.** `hud_style.c` gained:
  - the list entry (`5:sm2`), `HUD_STYLE_MAX` 5 and the dvar description;
  - ownerDraws 3003 and 3004, drawn at paint time through the CG_OwnerDraw hook it already shares with voice.c;
  - their three byte checks, and one more part of the register line: `health ok|differs, weapon icon
    ok|...|differs`.

  The pure parts (`HudHealthPermille`, `HudIconFit`) are in `hud_style_policy.h`, tested by
  `tests/hud_style_policy_test.c`. The option rows read `k33_hudStyleList`, so the HUD style drop-downs in
  `OptionView_video.as` and `MainPause.as` list SM2 without change. Its label `K33UI_HUD_STYLE_SM2` ("SM2" in both
  languages) is in `codol_*_ui`.
- **Live** (2026-11-13, own client copy `D:\CoD\k33_sm2_client`, throwaway profile `edtest`, Afghan bot Team
  Deathmatch, 1920x1080). Seen working:
  - the panel, clip / reserve after firing, the grenade count dropping to 0 with its icon dimming, and the weapon
    icon (Vector, MG4);
  - the top score with the enemy fill and the "losing" caption after a death (0 : 100), the emblem and the timer;
  - the name, and everything hiding while dead;
  - the HUD-style row of the pause menu showing "SM2";
  - `set k33_hudStyle 0` / `5` switching in the same frame.

  Second round (killstreaks, health, icon), with `codol_devServer 1` / `codol_dev_cheats on` over the host's rcon,
  `take health N`, frozen bots (`sv_botStop 1`) moved in front of the player (`dev_clientcmd bots setviewpos`):
  - the streak slot dim at 0 kills, the pips filling at 2 kills, then the UAV lit with its key at 3;
  - health full, at 30 % after `take health 70`, regenerating back to full within about 3 s, and red at 18 %;
  - the icon at its image's aspect: Vector 128x64, M4 128x32, AK-47.

  Not checked live: 720p and 4:3 (`r_mode` set in a running client did not change the 1920x1080 window), Search and
  Destroy / Sabotage (the kept stock items), free-for-all, an akimbo weapon, three or more chosen streaks (the
  test profile has only the UAV).

  Known observation, not investigated (not caused by the HUD styles): during the second round the client's
  `main\console_mp.log` had `PARITY ... snap-ps: playerState weapon 121 is past the last registered weapon 120 (its
  weapon configstring has not reached the client yet?)` right after the test player was given weapons with `give`.

## HUD sway (`cg_hudSway`, every style)

User request (16 November): the HUD moves with the view, the player's movement and nearby explosions, as the SM2
mod's HUD did, first for SM2, then for every style (stock 0, v26 2, v33 3, sm2 5).

- **Switch.** `cg_hudSway`, int 0 / 1, archived, **default 1** (on: the user asked for the HUD to move; 0 brings back
  the still HUD). Options > Game has a "HUD sway" row (Off / On) under HUD style; the pause menu's Options list has it as a stepped row under HUD style (docs/UI_STYLE_OPTIONS_20261112.md).
- **C** (`hud_style.c` `HudSwayTick`, main tick, before the frame's menus paint; pure part `hud_style_policy.h`
  `HudSwayStep`, tested by `tests/hud_style_policy_test.c`). Whenever the switch is on and a match runs (any style),
  it reads the predicted playerState from the same cg the HUD reads (`HudCg`): view angles cg + 0x14C, velocity
  cg + 0x28, health cg + 0x190; and the camera shake: the rendered view's angles cg + 0xB4418 (C1 copies the
  playerState's there at 0x0045648C, byte-checked, and adds its sin-driven explosion / damage shake at 0x0042C577..)
  minus the playerState's. It writes `cg_hudSwayX` / `cg_hudSwayY` (ints, hundredths of a 640x480 unit, read only).
  Off or out of a match it writes 0 once and then leaves the two alone, so a test can set them by hand.
  - turning: the HUD trails the turn by `HUD_SWAY_LOOK` 0.02 units per degree / second (at most 720 deg/s): turning
    right (yaw falling) moves it left, looking up (pitch falling) moves it down;
  - steps: a bob of `HUD_SWAY_BOB` 1.1 units at running speed (190 u/s), 1.7 steps a second;
  - jumps and falls: 0.01 units per u/s of vertical speed, at most 4, rising moves it down;
  - all three through a damped spring (16 rad/s, damping 0.55: a small overshoot), at most 12 units;
  - camera shake: 3 units per degree of shake, at most 8, passed straight through (it is already a motion);
  - dead: the HUD settles back.
- **Menus** (`tools/build_hud_styles.py` `sway()`, run on every item of the four hosts, in this zone and in the
  survival HUD's menufile: the stock items, the Survival panel's and every style's). An item with its own
  `exp rect X` / `Y` gets `( {its expression} + ( dvarint("cg_hudSwayX") * 0.01 ) )`, one without gets
  `( its rectClient x + ( .. ) )`. rectClient (+0x18) is the item's own position, which an `exp rect X` replaces
  (+4 is the laid-out rect: rectClient plus the menu's origin, scorebar_hd's y -20). With the dvars at 0 every item is
  where it was: live, the four styles' HUD at offset 0 matched the build without `sway()` pixel for pixel
  (minimap, ammo, score, streak column). `tests/test_hud_styles.py` checks that every item has exactly one sway on
  x and one on y and that removing it gives the item back (`strip_sway`). (`zonekit/menu_source.py`'s menuDef keyword
  `itemOffset ( x ) ( y )`, written for SM2 first, is still there and tested; sm2.menu no longer uses it, since
  `sway()` covers SM2 like the others.)
- **Live** (16 November, `D:\CoD\k33_sm2_compass_client`, profile `edtest`, co-op on mp_crash_tropical, 1024x768
  windowed, headless: commands through `k33_cmd_<pid>.txt`, the turns through the host's rcon
  `dev_clientcmd 0 setviewpos ..` at 90 Hz, pictures through `k33_shot.req`; the dvars were still named
  `k33_hudSway*` then, renamed `cg_hudSway*` later that day with no other change, and not re-run under the new names):
  - `cg_hudSway 0`, `cg_hudSwayX 1000`, `cg_hudSwayY 1000`: in every style the HUD moved 10 units right and down
    (16 px at 1.6 px/unit), ownerdraws included: SM2's minimap, compass strip, player arrow, clip count, weapon icon
    and health bar; stock's minimap and ammo; v26 / v33's minimap, ammo panel, score bar (scorebar_hd, the -20 origin)
    and kill / death bar; v33's streak column;
  - live turns, the ammo panel's offset: left at 180 deg/s +2 to +3 units, right at 360 deg/s -4 to -6, looking up
    at 40 deg/s +0.6 down, in all four styles; at rest 0. With the first gain (0.010) a 180 deg/s turn gave 1.2
    units: hardly visible, so the gain was doubled;
  - an instant 90 degree turn (one `setviewpos`) gives a one-frame kick of about 1 unit (a single frame of turn rate).
  - Not seen live: explosions. Co-op on Crash has no frag loaded; `codol_explodeprobe 0 c4_so` (developer 1) fired
    an explosion event 400 units overhead, with no visible effect or shake. `take health 40` gives no view kick.
    The shake term is the rendered view's offset from the playerState's angles, 0 at rest (measured), so it moves
    the HUD only when C1 shakes the camera.
- Seen on the way, not caused by the sway: under v33 in co-op the streak column's five slots draw as white boxes; the
  build without `sway()` draws them the same.
- **Not checked live:** walking bob and jumps (no headless movement: `+forward`, `+left` do nothing in an unfocused
  window), Survival, killcam / spectating.

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
- 3.3.3.3: `extract --styles v26,v33 --v33 DIR --images v33=VIEW`, where VIEW holds 3.3.3.3's client exe and a
  junction to its `IIPS` folder (as for v12; the install itself is never written). The dumper additionally needs
  `zone-materials` to read the 3.x Material layout (name at +0x10, count +0x50, technique set +0x58, table
  +0x5C) and 1.2.1.12 / 3.x material names in `MenuCommands.cs`. Set `CODOL_CACHE_DIR` to a private folder: the
  first run indexes 238 zones (about 5 minutes) and a cold `export-textures` can otherwise look hung.
- Adding or removing a shipped style changes three places that the test compares: the sources, `HUD_STYLE_LIST` in
  `hud_style.c`, and the expected list in `tests/test_hud_styles.py`.
- SM2: edit `sources/sm2.menu` and run `python tools/build_hud_styles.py`; no install or dumper is needed.

## Integration notes

- `src/kernel33.c`: `#include "hud_style.c"` after `theatre.c`, `InstallHudStyle();` after `InstallTheatre();`.
- `src/patch_locale_policy.h` `kCodolPatchZoneMerged`: three rows (`hud_styles_postgfx_mp`,
  `codol_english_hud_styles`, `codol_chinese_hud_styles`), required by tests/test_zone_packs.py. Both files are
  pinned by `ui_baseline.json`, which is not re-recorded here.
- `hud_style.c` takes one `K33OnMainTick` slot. A local-testing build then registers seven before Product Pi's
  on-demand one. K33_MAX_TICKS was 8 and is 12 since the language switch and this feature each took a slot.
- `zone_packs/codol_common_postgfx_mp.csv` has its new build md5 recorded; the two locale packs gained a part and
  were left unrecorded (they already differed from their recorded md5 before this change).

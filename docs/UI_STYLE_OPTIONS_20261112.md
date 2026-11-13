# Language, Button icons and HUD style rows (12 November 2026)

Three settings rows, in both places a player changes settings:

| Place | Movie / class | Where |
|---|---|---|
| Main menu, Options > Graphics | `codo.gfx`, `codo.views.option_sub.OptionView_video` | under the Graphics check boxes, left column |
| Pause menu, Options list | `codoingame.gfx`, `MainPause` (C1's `PauseMenuView` stays compiled stock) | under the last slider row (FOV, or Brightness in fullscreen), above Back to Lobby |

Each row reads and writes a dvar that Kernel33.dll owns. The movies only show
and set; what a value does (reloading strings, swapping glyphs, loading a HUD
zone) is native work on the DLL side.

This is source, compiled tags and a candidate build. Nothing here has been seen
in a running game: the dvars are written on other branches. See "Not verified".

## Dvar contract

Reads go through `CFunctionContainerCommon.Dvar_GetInt` / `Dvar_GetString`,
writes through `CFunctionContainerCommon.setdvar(name, String(value))`.

| Row | Set | Choices | Names |
|---|---|---|---|
| Language | `k33_language` (string, archived) | the ids of `k33_languageList`, `;`-separated (`en;zh`) | `K33UI_LANG_NAME_<ID>` |
| Button icons | `gpad_glyphs` (int, archived, 0..6) | 0 Auto, 1 Xbox, 2 PlayStation, 3 Wii U / Switch, 4 Generic, 5 Text only, 6 Keyboard & mouse | `K33UI_GLYPHS_AUTO / XBOX / PS / NINTENDO / GENERIC / TEXT / KBM` |
| HUD style | `k33_hudStyle` (int, archived) | the numbers of `k33_hudStyleList`, `;`-separated `number:id` pairs (`0:stock;2:v26`) | `K33UI_HUD_STYLE_<ID>` (`STOCK`, `V12`, `V26`, `V33`, `V35`) |

Also read: `k33_languageState` (0 idle, 1 switching, 2 saved and applies on the
next start), `k33_languageSerial` (moves when a live switch has finished), and
`k33_padinfo` (non-empty once the controller module is running: that is what
shows the Button icons row). `<ID>` is the id in upper case.

Rules both movies follow (one text, `k33Style*`, between the `k33 style shared`
markers of both sources; `tests/ui_style_rows_test.js` requires the two copies
to be identical and runs them):

- An empty or missing list means no row. An older Kernel33.dll without
  `k33_languageList` / `k33_hudStyleList` / `k33_padinfo` shows the menu as it
  was before. Blank, malformed (`x:bad`, `3`, `:v33`) and repeated entries are
  dropped. The row appears by itself if its list turns up while the menu is open.
- A name with no localize entry shows the raw id (`de`, `v99`).
- A current value the list does not have shows the first choice; nothing is
  written until the player picks.
- Stepping wraps at both ends. A list of one choice shows it and has nothing to
  step to.
- A pick is shown at once and set at once. The row then keeps that for 12 frames
  and after that shows what the dvar actually holds, read every 6 frames. So a
  value the engine refuses does not stay on show: an older DLL whose `gpad_glyphs`
  stops at 5 refuses 6, and the row returns to the stored choice. The same read
  follows a value changed from the console or from the other menu.
- The Button icons row's own text comes from `gpad_glyphs`, not from
  `k33_glyphSerial`, which does not move when the effective display is unchanged
  (0 to 6 while already on keyboard). The rows show no key or button hints, so
  there is nothing of theirs to ask again when `k33_glyphSerial` moves.

## Main menu: Options > Graphics

A label and a `<  name  >` stepper per row, built in code (as the Controller
tab's steppers are), at x 12..288, y 405..463 of the panel: below the check
boxes (which end at y 401), above the warning line (y 465), left of the texture
rows (x 296).

- Mouse: click the right half for the next choice, the left half for the previous.
- Gamepad: the menu cursor snaps to each half (`PadSnap.hint(.., SPLIT_H)`), and
  A clicks it.
- The rows apply immediately. They take no part in Apply, Cancel or Recommended:
  those handlers do not touch the three dvars, and the rows keep no pending state,
  so there is nothing for Cancel to undo or for Apply to repeat.
- `k33_languageState` 2 shows "Restart the game to apply the language" under the
  warning line (x 12, y 486); 1 shows "Switching language...".
- The rows are made again from the dvars every time the tab is shown, and are
  only polled while it is shown.

The rows are on Graphics, not on Controller, because `OptionView` (the Controller
tab's host) may only be changed through `tools/optionview-method-producer`; the
generic compile route refuses it. Keeping all three together on one tab was
preferred to splitting them. One consequence: C1 disables the Graphics tab while
the player is in a room or a ranked match, so there the rows are reachable only
from the pause menu once the match starts.

## Pause menu: Options list

Each row is another instance of the list's own slider row class
(`InGameMenuSliderBtn`), used as a stepped chooser: one stop per choice, the
choice's name where a slider shows its number. The bar is shortened so the
longest name of the row fits inside the slider's 195 px (never below 60 px). The
rows below move down and the list background grows, as for the FOV row.

- Mouse: click or drag along the bar for the nearest choice; a click on the row's
  label or on the name steps to the next choice.
- Gamepad: left / right step (wrapping); select steps forward. Up / down move
  between rows as before.
- `k33_languageState` 1 / 2 shows the same line under the list.
- A submenu without sliders (change team, leave confirmation, the theatre pause)
  gets no rows.

## What refreshes live, and what waits

C1's translator resolves `@KEY` when a text is set and hands the movie a copy, so
text already on screen does not change by itself. When `k33_languageSerial` moves:

| Text | Refresh |
|---|---|
| The three rows, both movies | live: made again, names and labels |
| Pause menu: the whole open list (class rows, Sensitivity, FOV, Brightness, Back to Lobby), the list header, Back to Game, the network-quality label and button, the class header | live: `PauseMenuView.gotoMenu(getCurrentMenu())` builds the list again and the fixed labels are set again. Pad focus stays on the row it was on. |
| Pause menu: the loadout panel beside the list | the same rebuild shows it again; the texts C1 sets each time it shows follow, any it set once do not |
| Options > Graphics: every label, check box and button of the tab, the Options window's title and four tab buttons | live (`k33Texts`, `k33StyleRelocalize`) |
| Options > Graphics: the combo boxes' lists | live when the tab holds nothing unapplied (the tab is filled again exactly as when it opens). With unapplied choices they are kept, only Max FPS's list is translated, and the others follow when the tab is next opened. |
| Options > Audio, Controls and Controller tab contents | not live. Their texts are set once when the movie loads. |
| Everything else in the front end (main menu, lobby, store, ...) | not live, for the same reason |
| The HUD (`codohud.gfx`) | not this change |

So in a match the pause menu follows a live switch completely. In the front end
only the Options window's Graphics tab and tab strip do; the rest of `codo.gfx`
shows the new language once the movie is loaded again (it is freed on match entry
and loaded on return, and at start). A complete live switch of the front end
needs the DLL to reload `codo.gfx` after the switch, or to report state 2 there.
A switch made while a menu is closed is picked up when it is next shown.

`k33_glyphSerial`: C1's key-name natives at `0x5B06CC`, `0x5B0746`, `0x5B091E`
and `0x5B09E9` return every device's keys whatever `gpad_glyphs` says, so a stock
menu label fed by them does not follow the override. None of the rows uses them.

## Localize keys

Nineteen keys, English and Chinese, in `ui_keys.py` `NAMED` and in
`zones/codol_{english,chinese}_ui/localize.str` with their manifests:
`K33UI_LANGUAGE`, `K33UI_GLYPHS`, `K33UI_HUD_STYLE`, `K33UI_LANG_NAME_EN`,
`K33UI_LANG_NAME_ZH`, `K33UI_LANG_RESTART`, `K33UI_LANG_SWITCHING`, seven
`K33UI_GLYPHS_*`, five `K33UI_HUD_STYLE_*`.

They are entered once per language, in `codol_*_ui`, and not again in
`codol_*_ui_extra` (where `codoingame.gfx`'s other keys live). Both string zones
are parts of the same `codol_<language>_locale` pack, which is loaded whole and
stays loaded in a match, so the pause menu sees them. The pack builder would drop
the second copy as shadowed anyway, and an entry that did load twice would take
two of C1's 12000 localize slots.

Each language is named in its own language. The English strings name Chinese as
`中文 (Chinese)`, so the row stays readable if the Latin font movie has no CJK
glyphs. A new language or HUD style needs only its `K33UI_LANG_NAME_<ID>` /
`K33UI_HUD_STYLE_<ID>` entries; until then its raw id is shown.

`python ui_keys.py` was not run (it rewrites `codo.gfx`'s `patch.txt`
wholesale); the entries were added by hand in `NAMED` order.

## Build, tests, gates

```
python build_zones.py --store-class controller_ingame:scaleform/default/codoingame.gfx:MainPause --store-class voice_ui_mp:scaleform/default/codo.gfx:codo.views.option_sub.OptionView_video
python build_zones.py --candidate voice_ui_mp controller_ingame codol_english_locale codol_chinese_locale
node tests/ui_style_rows_test.js
```

`tests/ui_style_rows_test.js` (in `tests/run_tests.sh`) runs the rows' actual
ActionScript from both sources against a fake dvar table: list parsing, missing
dvars, values not in the list, wraparound, a refused set, the hold and read-back,
a language serial change, a list that changes while open, and the nineteen keys
in both languages, once per pack.

`ui_baseline.json` was re-recorded for the changed `codo.gfx` files with
`python -m zonekit.ui_baseline --record`.

The four-tag composition gate (`tools/p2p-four-tag-composition`, exercised by
`tests/test_optionview_method.py`) is **not** succeeded and fails on this branch.
It proves that the checked-in `codo.gfx` is the reviewed selector movie with
exactly four qualified class tags changed, and pins the movie digest and the
bytes of `patch.txt`. `OptionView_video` is a fifth changed tag. Its successor
would be a new composition over the current one (the retained predecessor movie,
the prior `patch.txt`, the new tag's identity, and an independent semantic review
receipt like `actual-semantic-review.json.DATA`), wired in the way a618141 wired
this one into `playlist-selector-producer/current_endpoint.py`, then pinned in
`ui_baseline.json`. That receipt is a reviewer's decision and was not invented
here.

## Not verified

No running game was available with these dvars. Unverified:

- that the rows appear where described and nothing overlaps at every resolution,
  in both the multiplayer and the zombies / co-op pause view and both Options
  panel variants;
- text widths in the pause rows (the longest names against the 195 px slider);
- CJK glyphs for `中文` under the English font;
- that `setdvar` reaches the three dvars from Scaleform (archived / read-only
  flags on the DLL side decide that);
- the live re-localisation: that `gotoMenu(getCurrentMenu())` and re-setting the
  Graphics tab's texts show the new language once the DLL has switched strings.

## Manual test

1. Install the candidate packs with a DLL that has the dvars. Main menu > Options
   > Graphics: three rows under the check boxes. Click each half of each stepper;
   check `k33_language`, `gpad_glyphs`, `k33_hudStyle` in the console after each.
2. With a pad: move the cursor onto a stepper, confirm it stops on each half, press A.
3. Set `gpad_glyphs 3` in the console: the row follows within a moment.
4. Change Resolution without Apply, then switch language: the pending resolution
   is still selected, labels are in the new language. Press Cancel: the language
   stays changed.
5. Start a match, open the pause menu: three rows under FOV / Brightness, Back to
   Lobby below them, list background covering all rows. Drag a bar, click a
   label, step with the pad (left / right / select).
6. Switch language from the pause menu: every row of the list, the header and
   Back to Game change; pad focus stays on Language.
7. A language that needs a restart (`k33_languageState 2`): the line appears under
   the rows in both menus.
8. An older Kernel33.dll: both menus look as before, with at most the Button icons row.
9. Open change team / leave confirmation: no rows; back to the main list: rows return.

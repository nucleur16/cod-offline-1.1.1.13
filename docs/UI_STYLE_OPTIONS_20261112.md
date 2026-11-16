# Button icons and HUD style rows; the Language row, switched off (12 November 2026)

Two settings rows, in both places a player changes settings:

| Place | Movie / class | Where | Control |
|---|---|---|---|
| Main menu, Options > Graphics | `codo.gfx`, `codo.views.option_sub.OptionView_video` | under the Graphics check boxes, left column | a drop-down, the tab's own `CodoComboBox` |
| Pause menu, Options list | `codoingame.gfx`, `MainPause` (C1's `PauseMenuView` stays compiled stock) | under the last slider row (FOV, or Brightness in fullscreen), above Back to Lobby | a slider row used as a stepped chooser (that list has no combo box) |

Each row reads and writes a dvar that Kernel33.dll owns. The movies only show
and set; what a value does (swapping glyphs, loading a HUD zone) is native work
on the DLL side.

**The Language row is switched off** (owner, 12 November 2026: out of the options
menus until the asset limit of a live language switch is solved). One constant in
the shared block of both sources, `K33_STYLE_LANGUAGE_ROW = false`: the row and
its "Switching language..." / "Restart the game to apply the language" line are
not made, the other two rows close up, and no `k33_language*` dvar is read for a
row. The language is still changed from the console (`k33_language`,
`k33_setlanguage`), and a finished switch (`k33_languageSerial`) still translates
the remaining rows and the Graphics tab again. Setting the constant to `true` in
both sources (the text between the markers must stay identical) and storing the
two classes brings the row back; its localize keys are still in the string zones.
Everything below that is about the Language row describes that switched-on state.

What was seen in a running client (12 November 2026, a test client next to the
player's, English and Chinese, 1920x1080): "Seen in a client" at the end.

## Dvar contract

Reads go through `CFunctionContainerCommon.Dvar_GetInt` / `Dvar_GetString`,
writes through `CFunctionContainerCommon.setdvar(name, String(value))`.

| Row | Set | Choices | Names |
|---|---|---|---|
| Language (switched off) | `k33_language` (string, archived) | the ids of `k33_languageList`, `;`-separated (`en;zh`) | `K33UI_LANG_NAME_<ID>` |
| Button icons | `gpad_glyphs` (int, archived, 0..6) | 0 Auto, 1 Xbox, 2 PlayStation, 3 Wii U / Switch, 4 Generic, 5 Text only, 6 Keyboard & mouse | `K33UI_GLYPHS_AUTO / XBOX / PS / NINTENDO / GENERIC / TEXT / KBM` |
| HUD style | `k33_hudStyle` (int, archived) | the numbers of `k33_hudStyleList`, `;`-separated `number:id` pairs (`0:stock;2:v26;3:v33`) | `K33UI_HUD_STYLE_<ID>` (`STOCK`, `V12`, `V26`, `V33`, `V35`) |

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
- Pause menu: stepping wraps at both ends. A list of one choice shows it and has
  nothing to step to.
- Only the player's pick sets a dvar. What the code selects (filling a list,
  showing what the dvar holds) sets nothing: on Options > Graphics a combo box
  fires its change event for those too, and the tab ignores them
  (`_k33StyleSync`).
- A pick is shown at once and set at once. The row then keeps that for 12 frames
  and after that shows what the dvar actually holds, read every 6 frames. So a
  value the engine refuses does not stay on show: an older DLL whose `gpad_glyphs`
  stops at 5 refuses 6, and the row returns to the stored choice. The same read
  follows a value changed from the console or from the other menu.
- The Button icons row's own text comes from `gpad_glyphs`, not from
  `gpad_glyphSerial`, which does not move when the effective display is unchanged
  (0 to 6 while already on keyboard). The rows show no key or button hints, so
  there is nothing of theirs to ask again when `gpad_glyphSerial` moves.

## Main menu: Options > Graphics

A label and a drop-down per row, like the tab's other options. The drop-down is
one more instance of the class the tab's own combo boxes are: C1 places them by
class name (`codoComboBox_l2` on `option_videoPanel`, `codoComboBoxCyborgs` on
`option_videoPanelCyborgs`), so the tab takes the class of its `myCBAspectRatio`
(`getDefinitionByName(getQualifiedClassName(..))`, the way C1's `CodoList` makes
its item renderers) and `new` gives the symbol with its button, list, scroll bar
and skin. The box is made once per row, after the tab is on the stage (a
`CodoSprite` builds itself when it gets there), and kept.

- Place: labels at x 12 (the check boxes' left edge), boxes at x 98 (the Display
  Mode / Resolution column, the same 150-wide box), rows at y 407 and 437: below
  the check boxes (which end at y 398), above the warning line (y 467), left of
  the texture rows (x 298). The label sits by its box as the Graphics rows'
  labels do (`myTFSSAO.y - myCBSSAO.y`) and has their font, size and colour.
  With the Language row switched on there are three rows, at y 399 / 421 / 443.
- The list is `[{label, id}]`: the choice's translated name and the value its
  dvar takes. It opens **upwards** (`setDropDownAtRuntime(false)`: the rows are
  at the foot of the panel) and, like every combo box of the tab, comes to the
  front as it opens (`EVT_PRE_OPEN`), so it covers the check boxes above it.
  Seven Button icons choices in a five-row list: C1's scroll bar and wheel.
- Gamepad: nothing special. PadSnap finds a combo box's button
  (`comboBox_l2_button`) and, while the list is open, its rows, exactly as for
  the tab's other boxes; the controls the list covers stop being targets.
- The rows apply immediately. They take no part in Apply, Cancel or Recommended:
  those handlers do not touch the dvars, and the rows keep no pending state, so
  there is nothing for Cancel to undo or for Apply to repeat.
- A row whose list is empty is hidden (box and label) and the rows below move up.
- While a list is open its box is not moved to the dvar's value.
- `k33_languageState` 2 shows "Restart the game to apply the language" under the
  warning line (x 12, y 486); 1 shows "Switching language..." (Language row only).
- The rows are set again from the dvars every time the tab is shown, and are
  only polled while it is shown.

The rows are on Graphics, not on Controller, because `OptionView` (the Controller
tab's host) may only be changed through `tools/optionview-method-producer`; the
generic compile route refuses it. One consequence: C1 disables the Graphics tab
while the player is in a room or a ranked match, so there the rows are reachable
only from the pause menu once the match starts.

### The black boxes (fixed)

The first build drew each row's label as a line of small black squares. Cause:
the labels were code-made `TextField`s whose text was set to `"@K33UI_..."`.
C1's translator resolves `@KEY` as a text is set and gives the field the text
back in the field's *authored* format; a field made with `new TextField()` has
none, so the format set through `defaultTextFormat` was replaced by the default
(Times New Roman, 12, black), and a field with `embedFonts` has no glyphs for
that font: every character is drawn as a box. Measured in the client with a
diagnostic build: the label read back `size 12, colour 0` where `16, 7450297
($font_3)` had been set, with its translated text intact. The stepper next to it
was fine because its text was translated first (`LocalizeUtil.localize`) and
then set, which is also what `OptionView` and `OptionView_audio` do for their
code-made texts. So: a code-made field never gets `@KEY`; the label and the
language line are translated with `LocalizeUtil.localize` and then set. The same
line in the pause menu (`MainPause`'s note under the list) had the same fault
and is fixed the same way; the pause rows themselves use the row class's own
authored fields and were not affected.

## Pause menu: Options list

Unchanged: that list has no combo box component. Each row is another instance of
the list's own slider row class (`InGameMenuSliderBtn`), used as a stepped chooser: one stop per choice, the
choice's name where a slider shows its number. The bar is shortened so the
longest name of the row fits inside the slider's 195 px (never below 60 px). The
rows below move down and the list background grows, as for the FOV row.

- Mouse: click or drag along the bar for the nearest choice; a click on the row's
  label or on the name steps to the next choice.
- Gamepad: left / right step (wrapping); select steps forward. Up / down move
  between rows as before.
- `k33_languageState` 1 / 2 shows the same line under the list (Language row only;
  the line is not made while the row is switched off).
- A submenu without sliders (change team, leave confirmation, the theatre pause)
  gets no rows.

## What refreshes live, and what waits

C1's translator resolves `@KEY` when a text is set and hands the movie a copy, so
text already on screen does not change by itself. When `k33_languageSerial` moves:

| Text | Refresh |
|---|---|
| The rows, both movies | live: made again, names and labels (Graphics: the row labels and the boxes' lists are set again, the selection kept) |
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

`gpad_glyphSerial`: C1's key-name natives at `0x5B06CC`, `0x5B0746`, `0x5B091E`
and `0x5B09E9` return every device's keys whatever `gpad_glyphs` says, so a stock
menu label fed by them does not follow the override. None of the rows uses them.

## Localize keys

Nineteen keys, English and Chinese, in `scripts/ui_keys.py` `NAMED` and in
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

`python scripts/ui_keys.py` was not run (it rewrites `codo.gfx`'s `patch.txt`
wholesale); the entries were added by hand in `NAMED` order.

## Build, tests, gates

```
python scripts/build_zones.py --store-class controller_ingame:scaleform/default/codoingame.gfx:MainPause --store-class voice_ui_mp:scaleform/default/codo.gfx:codo.views.option_sub.OptionView_video
python scripts/build_zones.py --candidate voice_ui_mp controller_ingame codol_english_locale codol_chinese_locale
node tests/ui_style_rows_test.js
```

`tests/ui_style_rows_test.js` (in `tests/run_tests.sh`) runs the rows' actual
ActionScript from both sources against a fake dvar table: list parsing, missing
dvars, values not in the list, wraparound, a refused set, the hold and read-back,
a language serial change, a list that changes while open, and the nineteen keys
in both languages, once per pack. The language logic runs with the switch on.
With the switch as the sources have it (off): no Language row in either menu, no
gap, no `k33_language*` dvar read for a row. For Options > Graphics the tab's
build / show / change / frame code runs against combo boxes that behave as
`CodoComboBox` does (a new list selects its first entry; every selection fires
the change event): filling and following the dvar set nothing, a pick sets once,
an unlisted value shows the first entry and writes nothing, an empty list hides
its row, a serial change sets labels and list names again.

`ui_baseline.json` was re-recorded for the changed `codo.gfx` files with
`python -m zonekit.ui_baseline --record`.

The four-tag composition gate (`tools/p2p-four-tag-composition`, exercised by
`tests/test_optionview_method.py`) is **not** succeeded and fails on this branch.
It proves that the checked-in `codo.gfx` is the reviewed selector movie with
exactly four qualified class tags changed, and pins the movie digest and the
bytes of `patch.txt`. `OptionView_video` is a fifth changed tag (and the Local Play removal changed `MatchMiniRoomCreatorView`,
`MatchMPCTRoomCreatorView_mpcreator` and `codo.k33.LocalMatchControls` as well: docs/OFFLINE_LOBBY.md). Its successor
would be a new composition over the current one (the retained predecessor movie,
the prior `patch.txt`, the new tag's identity, and an independent semantic review
receipt like `actual-semantic-review.json.DATA`), wired in the way a618141 wired
this one into `playlist-selector-producer/current_endpoint.py`, then pinned in
`ui_baseline.json`. That receipt is a reviewer's decision and was not invented
here.

## Seen in a client

12 November 2026: a test client of this branch's diagnostic DLL and candidate
zones, started next to the player's own client (its own folder, profile and
process mutex; off-screen window; driven with `cl_fakeMouse`, the synthetic pad and
back-buffer screenshots). 1920x1080, windowed.

- Before the fix (Chinese): the three row labels are lines of black squares, the
  steppers' texts are fine.
- After, Chinese and English: `按键图标` / `HUD 样式` and `Button icons` / `HUD
  style` in the Graphics rows' font and colour, each with a drop-down in the
  Display Mode column; no Language row; nothing overlaps the check boxes, the
  texture rows or the warning line.
- The Button icons list open: five rows and a scroll bar, above the box, over
  the check boxes. A click on Xbox: the box shows Xbox and the DLL logs
  `gpad_glyphs 1` at once. `gpad_glyphs 4` in the console: the box follows.
- Synthetic pad: the two boxes are PadSnap targets like the tab's other boxes
  (`comboBox_l2_button/myBtn`, 172x30); A on the box opens the list, its five
  rows become targets and the covered check boxes stop being targets; A on a row
  picks it and sets the dvar.
- The zombies lobby's Options (`option_videoPanelCyborgs`, Chinese): the same two
  rows with that panel's own orange combo boxes and label colour.
- Pause menu in a match: multiplayer (English) and zombies (Chinese): Button
  icons and HUD style under FOV, Back to Lobby right below them, texts drawn.

Not seen: other resolutions and aspect ratios; an open list on the Cyborgs panel;
a list of more than seven entries; a language switch finishing while the tab is
open (this client's DLL reports "no live switch"); anything of the Language row
(it is switched off).

## Manual test

1. Install the candidate packs with a DLL that has the dvars. Main menu > Options
   > Graphics: Button icons and HUD style under the check boxes, each a drop-down;
   no Language row. Open each list and pick; check `gpad_glyphs`, `k33_hudStyle`
   in the console after each.
2. With a pad: move the cursor onto a box, press A, move over the list, press A.
3. Set `gpad_glyphs 3` in the console: the box follows within a moment.
4. Change Resolution without Apply, pick a HUD style, press Cancel: the HUD style
   stays changed, the resolution does not.
5. Start a match, open the pause menu: two rows under FOV / Brightness, Back to
   Lobby below them, list background covering all rows. Drag a bar, click a
   label, step with the pad (left / right / select).
6. `k33_setlanguage` in the console with the Graphics tab or the pause menu open:
   the rows' labels and names follow when the switch has finished.
7. An older Kernel33.dll: both menus look as before, with at most the Button icons row.
8. Open change team / leave confirmation: no rows; back to the main list: rows return.
9. With `K33_STYLE_LANGUAGE_ROW = true` only: three rows; `k33_languageState 2`
   shows the restart line under the rows in both menus.

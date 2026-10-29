# Survival native HUD

The original C1 `killstreaks_hd` and `scorebar_hd` menus are retained in
`zones/controller_ingame/menufile/survival_hud_visibility.original.json`.
They were resolved from 1.1.1.13 `patch_common_mp.ff`, `ui_mp/hud_720.txt`,
using the existing AssetExplorer MenuCommands loader and its C1 struct layout.

`tools/build_survival_hud.py` adds `g_gametype != "survival"` to the killstreak
menu and each original MP score-panel item. All 69 killstreak items and 33
score-panel items, bindings, materials, positions and original checks remain
intact. The score menu retains its common HUD visibility rules and receives
four Survival-only items from the authored later CoDOL menu. The conditions use
C1's existing dvar-string operator, without leaving a HUD setting latched when
switching to another mode.

The small additional MenuList is included in the existing `controller_ingame`
part of `codol_common.ff`, which precedes the stock common HUD definitions.
No other native HUD menu is replaced. The existing `scorebar_hd` registration
paints the replacement, avoiding a popup or an unregistered standalone HUD.
Native menu authoring and preservation tests pass; the built candidate reloads
through the C1 loader with zero warnings. Final in-game panel visibility remains
a separate acceptance check.

The retained 2.6.9.7 source contains the divider, singular/plural enemy counter
and numeric template. Conversion preserves their authored geometry and font
settings. C1 lacks the later divider material, so its own white material draws
the same one-pixel line. The numeric template reads the actual current credit
wallet, with its Y moved to the later panel's card-counter line to avoid overlap
with the enemy count. The later template's unknown stat operator is not claimed
to be the same credit implementation. Card-deck mechanics are not introduced.
The original wave and armor producers remain; the duplicate upper-right credit
element is removed. Authoritative GSC values supply `surHUD_enemy` and
`surHUD_credits` for the native menu.

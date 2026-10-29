# Survival native HUD

The original C1 `killstreaks_hd` and `scorebar_hd` menus are retained in
`zones/survival_hud_postgfx_mp/menufile/survival_hud_visibility.original.json`.
They were resolved from 1.1.1.13 `patch_common_mp.ff`, `ui_mp/hud_720.txt`,
using the existing AssetExplorer MenuCommands loader and its C1 struct layout.

`tools/build_survival_hud.py` adds `g_gametype != "survival"` to the killstreak
menu and each original MP score-panel item. All 69 killstreak items and 33
score-panel items, bindings, materials, positions and original checks remain
intact. The score menu retains its common HUD visibility rules and receives
four Survival-only items from the authored later CoDOL menu. The conditions use
C1's existing dvar-string operator, without leaving a HUD setting latched when
switching to another mode.

The small additional MenuList loads in `codol_common_postgfx_mp.ff`, after
`code_post_gfx` initializes C1's default material assets and before the stock
common HUD definitions. An initial placement in the earlier `codol_common`
pack failed the real startup test because material defaults did not yet exist;
that placement was removed. Existing in-match GFx movies retain their timing.
No other native HUD menu is replaced. The existing `scorebar_hd` registration
paints the replacement, avoiding a popup or an unregistered standalone HUD.
Native menu authoring and preservation tests pass; the built candidate reloads
through the C1 loader with zero warnings. Final in-game panel visibility remains
a separate acceptance check.

The retained 2.6.9.7 source contains the divider, singular/plural enemy counter
and numeric template. Conversion preserves their authored geometry and font
settings. C1 lacks the later divider material, so its own white material draws
the same one-pixel line. The numeric template reads the actual current credit
wallet, on a separate line below the enemy count. The later template's unknown stat operator is not claimed
to be the same credit implementation. Card-deck mechanics are not introduced.
The original wave and armor producers remain; the duplicate upper-right credit
element is removed. Authoritative GSC values supply `surHUD_enemy` and
`surHUD_credits` for the native menu.

## C1 load-order and asset ownership audit

Read-only disassembly of the installed 1.1.1.13 `C1.exe` confirms the stage
change, rather than only the offline menu decoder. `kernel33.c` appends
`*_postgfx_mp` zones to the five-zone `code_post_gfx` load at `0x5984D2`,
using permanent allocation flags 1. This runs after the renderer and stock
code/default assets exist, before the later `common_mp`/`patch_common_mp` HUD
loads. The `survival_hud_postgfx_mp` source part belongs to
`zone_packs/codol_common_postgfx_mp.csv`; `controller_ingame` retains only its
original pre-gfx movie timing.

C1 `Load_MenuDefPtr` (`0x4C17B0`) loads the authored menu through `0x4C12B0`,
then calls `0x4CAFF0`. That function passes asset type 26 to
`DB_LinkXAssetEntry` (`0x4CC390`) and writes the returned canonical menu header
back into the MenuList at `0x4CB021`. Thus the later stock HUD MenuList resolves
`killstreaks_hd` and `scorebar_hd` to the earlier authored definitions; a new
MenuList filename does not require new HUD registration.

The retained menus contain twelve distinct comma material references: the two
killstreak bars, eight scorebar bar/cap materials, `hud_teamcaret`, and `white`.
Some real definitions arrive with common assets. After code-post-gfx, C1 can
safely create their default entries. Crucially, those references do not remain
bound to separate permanent placeholders: an existing default entry has zone
byte zero, and the later real asset takes the `0x4CC6AE` -> `0x4CC0F0` path.
At `0x4CC2BF` C1 adopts the incoming zone ownership; for material type 5 the
copy handler at `0xCD5188[5]` is null, so `0x4CC2EB..0x4CC2FF` copies the real
material into the existing header address. The temporary incoming header is
then released, and `0x4CC6B3..0x4CC6B9` returns the existing canonical header.
The menu's saved material pointers therefore receive the real contents when
stock common assets load. Embedding duplicate stock materials, shaders or
images is unnecessary for this placement.

This is static evidence for startup ordering, canonical menu reuse and material
replacement. The parent installed the corrected two-pack build and restarted
its owned error client; successful boot and Survival/MP visual acceptance are
separate runtime checks, not inferred from this audit.

## Credits line correction after live feedback

The parent observed the permanent native panel in a real Survival match: the
new enemy/credit panel replaced the MP score display, and the MP killstreak
panel stayed hidden. The user confirmed the result was much better, then
reported that credits overlapped the current-enemy counter. This is acceptance
of the panel integration, not acceptance of the earlier text spacing.

The earlier credit row retained the later numeric template's text scale 0.63 at
Y -36, while enemy text uses scale 0.24 at Y -58. The 22-unit baseline gap was too
small for the larger credit lettering. Only the adapted credit item now changes:
text scale 0.24 matches the enemy label, and Y -30 gives 28 baseline units between
rows. Its blue color, X 8, font selection and bottom-screen anchor remain;
the enemy/divider, all original MP items and their geometry remain intact.
Focused tests check a conservative 64-unit font envelope plus 4 units for the
style7 outline/shadow across480â€“2160pixel heights. The rebuilt candidate needs
one final live spacing check; no deployment or game restart is part of this edit.

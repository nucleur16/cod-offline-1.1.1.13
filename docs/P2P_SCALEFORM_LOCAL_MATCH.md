# Local hosting in the match lobby

`InRoomBaseView` adds one owner-only button, LOCAL HOSTING: ENABLED/DISABLED,
to Multiplayer, Zombies and Survival match rooms. There is no settings panel.
Gameplay settings and bot configuration stay with the existing game lobby.
Campaign uses its controller's default local-hosting behavior and hides this
button. A short native state/error line appears below the toggle.

The helper `codo.k33.LocalMatchControls` publishes changed room context using:

    p2p_room <sessionMap> <gametype> <category>

Context publication does not start a disabled local host. Clicking the toggle
sends only `p2p_localhost 1` or `p2p_localhost 0`, according to the native
`ui_p2p_enabled` dvar. Native hosting follows room changes while enabled.
The helper never sends bot counts, ports, difficulty or process settings.

The room's map and gametype IDs are validated as lowercase identifier tokens.
Survival uses category/gametype `survival`, Zombies `zm`, and ordinary MP uses
the selected room gametype with category `mp`. Ownership changes and leaving
the room hide the button. Both helper and room-base sources are handwritten
entries in zonelist.py so decompilation preserves them.

Build commands (Kernel33-consol), serialized with other store-class operations:

    python build_zones.py --store-class voice_ui_mp:scaleform/default/codo.gfx:codo.k33.LocalMatchControls
    python build_zones.py --store-class voice_ui_mp:scaleform/default/codo.gfx:codo.views.InRoomBaseView
    python build_zones.py codol_common_ui_mp --candidate --out output_lobby_local

`node tests/local_match_controls_test.js` executes the actual ActionScript
handler bodies with native/display stubs. It checks room-context publication
without startup, enabled/disabled toggle commands, category routing, owner-only
scope, invalid tokens, Campaign exclusion, and absence of settings arguments.
Graphical acceptance across all room types and human gameplay remain separate
from these handler, compilation and native startup checks.

## Layout verification (2026-10-28)

The old screen coordinates (900,790) collided with MP's Change Team button
and Zombies' player list. A first adjustment to the hidden room-chat title
was also wrong: the separate global Rooms chat shell covers that area.
The layout fixture now includes that shell, not only the room timeline.

Stock ChattingMainGroupView opens at (GameInfo.PANEL_GAP=50,876); its child
bounds are (0,-316)-(284.04,-7.55). The global chat therefore occupies
(50,560)-(334.04,868.45) in the common 1280x960 stage.

The compact toggle follows `myInputTF.x` and `myChatTitleTF.y - 110`.
Shipping MP/Survival uses (33,452) and Zombies (32,451). Including the border
and optional status, their bounds are (32.5,451.5)-(293.5,530) and
(31.5,450.5)-(292.5,529): at least 30 stage pixels above global Rooms chat.
No existing control is moved, resized or hidden by this fix. The in-match
HUD is untouched. In non-shipping Zombies, the optional developer panel
occupies the left pane; only the toggle moves right of that panel's actual
bounds when they intersect. Shipping builds keep the normal left anchor.

`node tests/local_match_layout_test.js` executes the actual room initializer
with extracted stock timeline bounds. It verifies unchanged stock positions,
clearance from global chat, known visible room controls and both top/bottom
Start/Ready layouts, across MP/Survival and Zombies in shipping/developer
configurations and eight viewport sizes (4:3,16:9,ultrawide; letterboxed and
stretched scaling). Regression checks reproduce both prior collision locations.
All four stock fonts and four patched English fonts fit the 244x26 caption
field: maximum text width190.81px, maximum line height18.65px at14px, with
TextField padding included in the assertions.

The fixture records the stock SWF's SHA-256 and can be regenerated:

    ffdec-cli.exe -swf2xml stock.swf stock.xml
    python tests/extract_room_layout.py --xml stock.xml --swf stock.swf --game <game> --out tests/fixtures/match_room_bounds.json

`stock.swf` is patch_ui_mp:scaleform/default/codo.gfx extracted with
zonekit.stock and its GFx signature replaced with FWS. Global chat positions
come from independently exported ChattingMainGroupView/GameInfo constants.
These are static geometry and handler checks; live C1 appearance is checked
by the parent session when the game is available.

# Local hosting in the match lobby

November 4 local candidate: the owner has a `LOCAL PLAY` checkbox immediately
above the stock bottom Ready control in Multiplayer, Zombies and Survival rooms. It is
hidden in Campaign and playlists. Selecting it highlights the button and
starts the embedded host; Ready waits for that host. With Local Play off,
Ready follows the dedicated route. The checked square indicates selection;
`STARTING...` or `ERROR` appears beside the label while native state changes.
Gameplay settings remain
in the stock lobby.

The helper `codo.k33.LocalMatchControls` publishes changed room context using:

    p2p_room <sessionMap> <gametype> <category>

Context publication does not start a disabled local host. Clicking the checkbox
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
    python build_zones.py voice_ui_mp --parts --candidate --out output_lobby_local_parts --game <approved Host copy>

The room-view class remains at its already compiled tag. FFDec currently
rejects its existing protected `getSyncDP` call when recompiling it; the new
placement therefore lives in the helper's `placeAboveReady()`.

`node tests/local_match_controls_test.js` executes the actual ActionScript
handler bodies with native/display stubs. It checks room-context publication
without startup, enabled/disabled toggle commands, category routing, owner-only
scope, invalid tokens, Campaign exclusion, and absence of settings arguments.
Graphical acceptance across all room types and human gameplay remain separate
from these handler, compilation and native startup checks.

## Layout verification (2026-11-04)

The live screenshot showed the previous 144x61 button next to Ready and the
user requested a checkbox above it. The new control is a single-line 180x13
click target with a 12x12 square. It anchors to `myForce_0.x + 100` and
`myInputTF.y + 18`. Extracted 1.1.1.13 room bounds place its full text/outline
at (1024.5,861.65)-(1205.5,876.65) in Multiplayer/Survival and
(1019.5,858)-(1200.5,873) in Zombies. It clears the stock room controls and
the bottom Ready control, whose top is at y=877. The handler and layout tests
pass across eight viewport sizes. Compiled local candidate `voice_ui_mp.ff`
SHA-256: `09b229e53b46c97c878e39e1a6a0cc57d8b66a454f3ee287c3370ac46312212f`.
The existing C1 processes still have the prior zone loaded; live visual and
click acceptance of this candidate require restarting the approved local paths.

## Previous layout verification (2026-11-03, superseded)

The helper repositions itself after the stock room view adds it. It anchors
to `myForce_0.x - 47` and `myInputTF.y + 33`, leaving the stock Ready control
untouched. Extracted 1.1.1.13 SWF bounds place the 144x61 button at
(878,877.65) in Multiplayer/Survival and (873,874) in Zombies. It clears the
measured stock controls and is 2-8 pixels left of bottom Ready.

The button and layout tests pass across eight viewport sizes. The rebuilt
local `voice_ui_mp.ff` SHA-256 is
`0d454eeff0e19347111d9392d048f27e4a74746499d91b09d96497db15ce4fc9`.
The task-owned Host and Guest copies carry that zone. Live visual/click
acceptance is still a separate gate.

## Previous layout evidence (2026-10-28, superseded)

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

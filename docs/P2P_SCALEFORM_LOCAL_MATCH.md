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

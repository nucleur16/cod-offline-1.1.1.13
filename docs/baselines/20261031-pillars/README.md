# Pillars map/mode compatibility for the r40 client

Pillars (`mp_sd_jardin`) is offered for Search & Destroy (`sd`) only. Existing
generic browser/Any providers retain their inventory; concrete MP creators,
manual Combat Training creation and host room edits receive compatible maps.
Mode changes preserve the selected map by ID when possible, then select an
existing compatible map. The active MP/CT tab owns its shared carousel, and
inactive handlers ignore arrow events. Outgoing create, Quick filter, room
edit, Ready and Start handlers refuse incompatible pairs. Cancel remains
native. An existing incompatible room retains its actual map card and has no
falsely selected SD mode; an explicit host selection repairs it.

Boot Camp remains Killhouse. Bot Training keeps its configured rotation and
native next-entry previews. No native coordinator, booking protocol, training
playlist, ticker, language-specific movie or room-name renderer was changed.

The baseline is cumulative client
`9bdaed789a383e4726c2dabf017bb0c38a120b15`. The previously unpatched CT creator
source was exported from that actual movie's tag5188. Compilation uses the
existing `zonekit.gfx.store_class` path against each stock class, and stores
five replacement tags with their source digests in `patch.txt`.

New immutable artifact: `client_shipRetail/patch/voice_ui_mp.ff`, 2,073,283 bytes,
SHA256 `da933c26884c72288e8911430c46fb6b34618aab0baa4e85c0c9bdc43016c3d3`.
Its embedded movie equals the source movie, SHA256
`88b736a3c7976524a1af19dc96fce401b9f2ee88e1af920b387de6ab51718ff9`.
The prior R2 artifact SHA256 `5c0ea2927887c888c7474b0c15a9917d3084863257e42c364cf006ebbccb1a90`
was retained unchanged. Only the shared movie asset differs in the zone.

`compiled-audit.json` records the actual before/after comparison: 5 changed
tags of 6,865; all other tags, including RoomInfo6559 and the ticker, are
byte-identical. Across the five exported classes, all 268 unrelated methods
retain identical tokens, including quoted string literals. Only the listed
26 methods were changed or added. No native C source changed.

Offline verification:

```
python -m unittest discover -s tests -p "test_*.py"
python tests/test_map_mode_compatibility.py
```

Repository discovery runs20 tests:19 pass and1 explicitly skips the private
x86 proof requiring its separately selected image/fixture. The new test runs
50 cases and61 control events against the actual source bodies. Set
`MAP_MODE_AS_ROOT` to the five-class FFDec export directory to run the same
cases against the compiled movie; that also passed. Controls emulate the
observed native GFx component events: combo index setters dispatch even for
the same index; Album data/index setters do not dispatch arrow events. These
are offline fixtures, not actual client gameplay.

The intentional MatchController ABI recompile is documented in the invitation
contract's `reviewed_class_update_20261031`, preserving the complete earlier
contract and three preexisting stale body pins. All166 current bodies are
pinned, including the disabled invitation producer. The invitation fixture
continues to pass168 checks. The shared UI baseline protects the changed
sources/tags and preserves all other pins.

Release remains held for the combined owner. Pair this UI with the map owner's
fail-closed mode/admission correction; do not introduce SD2 or publish a UI-only
rollout. No clients were launched for this contribution. Final r40 gameplay,
shipping/native/backend/catalogue/package qualification and production BOTH-mode
entry/completion/return/repeat/cancel/name/cap checks remain required. Earlier
R2 gameplay evidence does not qualify this new artifact. An empty compatible
inventory is not claimed as a visually accepted creator state.

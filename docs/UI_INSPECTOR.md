# Live UI inspector

The GUI uses PyQt5 (installed in the development Python here); headless reports use the standard library and existing zonekit. Run `python tools/ui_inspector.py` from the client repository, or pass a source
path. This is a read-only development tool; it does not patch or control the game.

Open the current HUD directly:

```powershell
python tools/ui_inspector.py zones/survival_hud_postgfx_mp/components/survival_status.json
```

The inspector supports brace-form `.menu` sources, decoded C1 MenuList JSON and
AssetExplorer `zone-menus` dumps. Serialized `.bin`/binary `.menu` files use their
decoded `.json` sidecar. Unexpanded macros/includes and unsupported directives
are reported, rather than silently presented as complete game behavior.

Choose a screen size, edit the JSON preview dvars and apply them. Menu and item
visibility and localized text expressions are interpreted without executing
scripts. Click an item to inspect its source, coordinates and expressions.
Bounds highlight text intersections using the actual preview font metrics.
Source saves reload automatically on a 500 ms poll. Reports can also be produced
without opening a window:

```powershell
python tools/ui_inspector.py SOURCE.json --report REPORT.json --state DVARS.json
```

The menu canvas approximates C1 screen placement and uses a system font. It is
useful for geometry, dvar visibility and collision checks, but is not a pixel
accurate replacement for C1's font rasterizer. Missing material textures use
visible placeholders; native ownerdraw and unresolved expressions are diagnosed.
Event handlers are shown as source only and are never executed.

For Scaleform, open a `.gfx`/`.swf` or select **Open GFx patch** and choose a
`.gfxpatch` directory. **Play GFx** runs the installed Scaleform SDK player in
its own interactive window. The genuine player supplies timeline animation,
ActionScript, mouse input and vector rendering. Logs remain visible in the
inspector. The default SDK is the local GFx3.3 D3D9 player; override it with
`--player PATH` if another SDK player is needed.

Patch previews reconstruct the real movie through `zonekit.gfx`, verify the
stock movie hash, and compile changed recorded ActionScript classes through the
installed FFDec CLI. Original game zones are read through the existing C1 zone
reader (`--game PATH`). The worker runs outside the GUI thread. Source saves
rebuild and relaunch only the inspector's owned player; this resets movie state.
No game process is stopped or written. FFDec and the SDK are local dependencies
and are not copied into the repository.

Standalone playback does not implement CoD's `ExternalInterface` callbacks,
live lobby/game data or its external texture/font loading services. A movie
depending on those services may render partially or report errors; the log is
evidence of that limitation, not successful in-game verification. External image dependencies are staged beside the preview movie from the installed game assets.
Use a self-contained exported movie or supply dependencies for more complete
standalone playback. Final in-game checks remain necessary for those behaviors.

Close the inspector to stop its owned player. Source/build failures retain the
last menu preview and display the error; they do not install files into the game.

## Code-driven navigation and screenshots

Desktop input is unnecessary. Menu screenshots render offscreen, without opening
a window or moving the cursor:

```powershell
python tools/ui_inspector.py SOURCE.json --menu scorebar_hd --state DVARS.json --resolution 1920x1080 --snapshot preview.png
```

The companion `preview.report.json` records evaluated state, unresolved features
and text overlaps. The renderer loads a real font file for offscreen output.
The regression renders the original oversized credit line and detects its
overlap, then verifies the corrected source has separate rows and real blue
glyph pixels.

Scaleform image dependencies are extracted from installed IWD/IWI assets using
the existing local AssetExplorer decoder. Actual image provenance and hashes
are reported. All72 external images from the stock CoDOL HUD resolved in the
local test; no placeholder was counted as a resolved image.

Movie navigation also runs through code, without desktop mouse/key events:

```powershell
python tools/ui_inspector.py MOVIE.gfx --capture-gfx obj/movie_capture --scenario actions.json
python tools/ui_inspector.py MOVIE.gfx --capture-gfx obj/component_capture --capture-backend ffdec --character pauseMenuView
```

An SDK scenario is a JSON array such as
`[{"op":"advance","seconds":0.5},{"op":"click","x":120,"y":160},{"op":"capture"}]`.
Available actions are `advance`, `frame`, `click`, `move`, `key`, and `capture`.
They are delivered to the movie's internal SDK AutoTest interface. The SDK4.3
GL player runs on an owned private desktop that is never switched to; this
prevents its startup focus behavior from affecting the user's visible desktop.
No physical cursor or desktop keyboard events are sent. Each run gets a fresh
capture directory, logs, image metadata and `capture-report.json`.

`--capture-backend ffdec` explicitly requests static frame/component rendering;
it rejects mouse/key scenarios because it does not execute ActionScript.
`--character` accepts an exact SymbolClass name or a numeric DefineSprite ID and
requires this static backend. The real CoDOL `pauseMenuView` component (ID211)
rendered its authored panels with67 real image dependencies and zero missing
images. Dynamic text requires CoD's initialization callbacks. Real SDK capture
also produced the CoDOL HUD root's uniform gray frames; those are recorded as
uninitialized content, not accepted HUD screenshots. The SDK sample control
produced visible nonuniform frames, proving the capture renderer itself works.

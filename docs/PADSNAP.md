# Menu cursor snapping (branch `feat/pad-snap`)

In the front end the pad's left stick drives the mouse cursor (CONTROLLER.md section 5). With snapping the cursor
knows where the controls are, the way console pointer UIs do it (Destiny, IW4x's pad cursor): it slows down over
buttons, settles on the one under it when the stick is let go, and jumps from control to control with the D-pad
or a flick. The in-match pause menu keeps its focus navigation (CONTROLLER.md section 8); nothing here runs in a
match.

Sources: `padsnap.c` (Kernel33, included after `controller.c`, which calls it from five marked places),
`zones/voice_ui_mp/rawfile/scaleform/default/{codo,codologin}.gfx.gfxpatch/as/codo/k33/PadSnap.as` (the target
publisher, one source compiled into both movies), `.../as/components/CodoUIComponent.as` (one line that starts it),
`.../codo.gfx.gfxpatch/as/codo/views/OptionView.as` (the two Options rows).

## 1. Where the controls are

C1's GFx has no API that lists buttons (a native hit test answers yes / no for one point), so the movies say it
themselves. `codo.k33.PadSnap` is a new class in both front-end movies (`build_zones.py --store-class`, below).
`components.CodoUIComponent`, the base class of every C1 control, starts it from its class initializer
(`private static var k33PadSnap:Boolean = PadSnap.boot();`): the player initialises that class when the movie
builds its first control, in codo.gfx and in codologin.gfx, and again whenever the movie is reloaded (after a
match). A DoABC tag that is not lazy is *not* run by C1's Scaleform at load, so the class cannot start itself.

While Kernel33 sets `k33_snapscan` (the pad is the last-used device and was used in the last 20 s, menu mode),
PadSnap walks the display list ten times a second (about 150-750 objects, 1-12 ms) and keeps every object that is
visible (alpha > 0, every ancestor visible), not clipped away by a mask or scrollRect, and is a control:
- a SimpleButton, an input TextField, a Sprite in buttonMode, or any InteractiveObject listening for CLICK /
  MOUSE_DOWN / MOUSE_UP / DOUBLE_CLICK (list rows, tabs, check boxes, dropdown entries, scroll bars, the Options
  rows Kernel33 builds). It counts with mouseEnabled off too when its children take the click (C1's components set
  mouseEnabled false on themselves; a slider's track is a child);
- not disabled: C1's BaseButton `enable` false (locked "Unlocks at level N" buttons) or `enabled` false;
- not a drag surface: an area over 8 % of the stage that only listens for presses (the 3D character view);
- C1's slider (`components.ProgressBar`, draggable, knob `myPointIco`): its point is the knob, so A does not jump
  the value;
- the bounds are the control's hitArea when it has one (BaseButton's `myHitArea`).

Then each candidate must be **reachable**: a click at its point must go to it. PadSnap decides like Flash does:
the topmost visible object under the point (`getObjectsUnderPoint`, shape hit test), then the outermost ancestor
with mouseChildren off, else the nearest mouse-enabled interactive ancestor; a mouse-disabled interactive object is
transparent (the click goes to what is below). A hitArea takes the click where it covers the point when nothing
above the control took it and something of the control is drawn there. The centre is tried, then 25 % / 75 % across.
So a popup or a dimmed page hides what lies below it. With a dialog up (C1's modal mask, `$layerMask`, in its page
layer) only controls of that layer drawn above the mask count, so the social side bar beside a dialog is skipped.

Published (stage units; one dvar holds about 1 KB, so in chunks): `k33_snapn` "seq targets chunks stageW stageH
mouseX mouseY ms nodes hidden authoredW authoredH how", `k33_snap0..7` "x y w h px py kind" comma separated (kind 0
button, 1 text field, 2 slider knob, 3 / 4 the halves of a "< value >" stepper, 5 a list the mouse wheel scrolls,
whose `px` is a signature of the text it shows). Kernel33 maps stage units to client pixels: the stage is the
visible area around the authored 1280 x 960, scaled to the client height and centred (a 16:9 window shows
1706 x 960 stage units, x from -213); the published stage mouse position lands on the cursor (checked in every
`gpad_snapdump`).

`PadSnap.hint(control, PadSnap.SPLIT_H)` marks a control whose click side matters (Options > Controller's
"<  value  >" steppers: the left and right halves are two targets); `PadSnap.SKIP` never makes one a target.

## 2. How the cursor moves

| input | what happens |
|---|---|
| left stick, partial push over a button-sized control | friction (down to 1 - 0.55 x strength of the speed) and a pull toward the control's point across the direction of travel (keeps the cursor in the row it moves along; never against the push) |
| left stick, full push / over a large card | normal speed, no pull, so a fast pass is not bent |
| stick let go | the cursor glides (45 ms) onto the control under it, or one within 8 + 40 x strength px (x client height / 960); past every control, back onto the control it passed in the last 120 + 260 x strength ms (the overshoot of a thumb's reaction time) |
| flick: a push under 220 ms that reached 0.75 | jump to the next control in that direction from where the push started (within 25 degrees of an axis it is that axis) |
| D-pad | the same jump; held: repeats after 380 ms every 150 ms |
| up / down past the last visible row of a list the wheel scrolls | one wheel notch at the cursor; when the next target list shows the list's text changed, the next row is under the cursor and the cursor settles on it; when it did not (the list's end), the cursor leaves the list in that direction |
| A | left mouse button at the cursor, as before; while held the cursor moves freely (drag sliders and scroll bars) |

Jump target ("next control in a direction"): the candidates whose point lies ahead within 60 degrees; score = the gap
to the control's rectangle along the direction + 2 x the gap across it (0 when the rectangle spans the cursor's
line, so the next row of a grid wins over a nearer-looking control off to the side) + 0.25 x the point's offset
across. Of several controls containing the cursor a clearly smaller one wins (a button in a list row), else the
one whose point is nearest (overlapping side-bar buttons).

Every move is a real cursor move (SetCursorPos with the window in front, WM_MOUSEMOVE to a test client), so each
control shows its own hover state. Without a target list (no movie, nothing published, or a list older than 1.5 s)
the free cursor is unchanged. Keyboard / mouse: the movie publishes only while the pad is the last-used device;
the cursor moves only on stick or D-pad input and the settle that follows it; a cursor moved by anything else ends
the settle and a pending jump.

## 3. Settings

| dvar | default | |
|---|---|---|
| `gpad_menu_snap` | 1 | archived; Options > Controller "Menu cursor snap" (菜单光标吸附) |
| `gpad_menu_snap_strength` | 0.6 | archived; 0..1, friction, pull, settle reach and overshoot memory; 0 = D-pad / flick jumps only; Options > Controller "Snap strength" (吸附强度), steps of 10 % |
| `gpad_menu_snap_debug` | 0 | 1 decisions (jumps, settles, flicks, wheel) to the console and Kernel33.log, 2 also every stick step and target list |

Commands: `gpad_snapdump` (the targets in client pixels with their classes, what covers each hidden one, what is
under the mouse or under `k33_snapprobe "x y"` stage units), `gpad_snapcount [reset]`. Test clients only:
`gpad_snapfake <lx> <ly> <ms> [buttons]` (the synthetic pad pushes for ms, then stays connected at rest like a
real pad) and `gpad_snapgoto <x> <y> <w> <h> [reaction ms]` (the player model below).

New localize keys (ui_keys.py NAMED, English + Chinese): `K33UI_PAD_SET_SNAP`, `K33UI_PAD_SET_SNAPSTR`.

## 4. Build: classes the stock movie does not have

`build_zones.py --store-class ZONE:RAWFILE:CLASS` compiles `<rawfile>.gfxpatch\as\<package>\<Class>.as` with
FFDec and stores the tag (`tags/`, the `new` / `as` / `sha256` lines of patch.txt), so builds need no FFDec. FFDec
only compiles over an existing class, so a new class is compiled over a donor class in a scratch copy and its code
is put in a new DoABC tag ahead of the SymbolClass tag (`zonekit/gfx.py` `store_class`). A class already stored
keeps its tag file name. `ui_keys.py` leaves such added classes alone; both new classes are in zonelist's
`handwritten` sets.

## 5. Verified (2026-10-23, k33-fovtest copy, synthetic pad, English and Chinese)

Background test client, `gpad_snapfake` / `gpad_snapgoto`, Windows Graphics Capture screenshots. Targets found and
reached with the D-pad and A:
- first-run key layout (both layout cards, OK), tutorial prompt (Start / Skip);
- main menu (three mode cards, Survival, Theatre, gear, power, social bar, language bar);
- multiplayer lobby (challenge rows, tabs, Refresh, Start, back, top icons; locked buttons and the 3D character
  view skipped), matchmaking screen (playlist rows), multiplayer room (team list, chat input, send, Ready);
- zombies lobby and zombies create-room (map arrows, room name / password fields, Map dropdown, difficulty,
  Create Room); the open Map dropdown: rows and its scroll bar, the D-pad walked down the rows, scrolled the list
  twice at its last row and left it at the end;
- Theatre screen;
- Options: Video (8 dropdowns, 9 check boxes, the FOV slider by its knob, Recommended / Apply / Cancel, tabs,
  close), Controller (Pad Layout / Button Binds, every setting row, the steppers' halves: A on the right half
  raised Snap strength 60 -> 70 -> 80 %, on the left half lowered it); everything under the Options window hidden;
- the quit dialog (Resume / Quit only; the main menu and the social bar hidden).

Not reachable here: the login screen (codologin.gfx carries the same publisher, but the offline lobby logs in
without it); the multiplayer Create Room map list (locked below level 5 for the test account; the zombies one was
used). In the quit dialog the test client's virtual cursor stops moving C1's pointer (the stage mouse stays where
the dialog opened; not checked with a real mouse), but clicks land at the snapped point.

### Measurements

A player model on the synthetic pad (`gpad_snapgoto`): a full push aimed once at the target, let go 180 ms after the
cursor is over it (a thumb's reaction time), then 0.4 pushes steered every 200 ms until the cursor rests on it.
`gpad_menu_cursor_speed` 1100 (the default), strength 0.6, 2560 x 1440 client.

| from -> to | free cursor: pushes, time | snapping: pushes, time | D-pad presses |
|---|---|---|---|
| main menu: MP card -> Zombies card | 1, 0.50 s | 1, 0.70 s | 1 |
| MP card -> Options gear (65 x 53 px) | 2, 2.63 s | 2, 2.42 s | 1 |
| MP card -> Survival | 2, 2.19 s | 1, 0.98 s | 1 |
| MP card -> Theatre | 2, 2.19 s | 1, 0.98 s | 2 |
| Zombies card -> power button | 2, 1.84 s | 1, 1.05 s | 2 |
| Survival -> social bar Party (screen edge) | 1, 1.56 s | 2, 2.92 s | - |
| Options: Video tab -> Apply | 2, 3.11 s | 1, 1.44 s | - |
| Video tab -> Controller tab | 2, 2.23 s | 1, 0.89 s | - |
| Apply -> Glow check box | 2, 2.56 s | 2, 3.72 s | - |
| Display dropdown -> FOV knob | 2, 2.34 s | 1, 0.63 s | - |
| close X -> Shadows check box | 2, 2.91 s | 1, 1.23 s | - |
| **total** | **20 pushes, 24.1 s** | **14 pushes, 17.0 s** | |

With the D-pad no aiming is needed at all: one press per control (Options > Controller: tab -> any setting row in
at most 6 presses, one per row).
